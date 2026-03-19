/**
 * @file test_button_callback_improved.c
 * @brief Enhanced unit tests for button callback functionality
 * 
 * This test file demonstrates comprehensive testing of button press functionality
 * by testing both the callback in isolation AND the full button press flow.
 * 
 * Testing Strategy:
 * 1. Test callback in isolation (what you currently have)
 * 2. Test that student_main() responds to simulated button presses
 * 3. Verify LED_STATE changes correctly
 * 4. Test edge cases (rapid presses, state verification)
 * 
 * Key Insight: We can run student_main() in a separate thread and simulate
 * button presses by calling the callback, then verify the results.
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "test_button_callback.h"

/* Thread for running student's main code */
#define STUDENT_MAIN_STACK_SIZE 1024
#define STUDENT_MAIN_PRIORITY 5

K_THREAD_STACK_DEFINE(student_main_stack, STUDENT_MAIN_STACK_SIZE);
static struct k_thread student_main_thread;
static k_tid_t student_main_tid;

/* Flag to track if student_main is running */
static volatile bool main_is_running = false;

/* External reference to student's main (renamed by CMake) */
extern int student_main(void);

/**
 * @brief Wrapper to run student_main in a thread
 */
static void student_main_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    main_is_running = true;
    student_main();
    main_is_running = false;
}

/**
 * @brief Test fixture setup - runs before each test
 */
static void test_before(void *fixture)
{
    ARG_UNUSED(fixture);
    
    /* Initialize the event */
    k_event_init(&button_events);
    
    /* Clear any existing events */
    k_event_clear(&button_events, BUTTON_EVENT);
    
    /* Reset LED state */
    LED_STATE = LED_OFF;
    
    /* Ensure no student_main thread is running from previous test */
    if (main_is_running) {
        k_thread_abort(student_main_tid);
        k_msleep(50);  /* Give time for cleanup */
    }
}

/**
 * @brief Test fixture teardown - runs after each test
 */
static void test_after(void *fixture)
{
    ARG_UNUSED(fixture);
    
    /* Clean up student_main thread if still running */
    if (main_is_running) {
        k_thread_abort(student_main_tid);
        k_msleep(50);
    }
}

/******************************************************************************
 * PART 1: Test callback function in isolation
 * These tests verify the callback works correctly without running main()
 ******************************************************************************/

/**
 * @brief Test that callback posts BUTTON_EVENT when called
 * 
 * This is the fundamental test: does the callback do its job?
 */
ZTEST(button_press_tests, test_callback_posts_event)
{
    /* Verify event is clear initially */
    uint32_t events_before = k_event_wait(&button_events, BUTTON_EVENT, 
                                          false, K_NO_WAIT);
    zassert_equal(events_before, 0, 
                  "BUTTON_EVENT should not be set initially");
    
    /* Simulate button press by calling callback directly */
    button_test_callback(NULL, NULL, BIT(0));
    
    /* Small delay to ensure event is posted */
    k_msleep(5);
    
    /* Verify event was posted */
    uint32_t events_after = k_event_wait(&button_events, BUTTON_EVENT, 
                                         false, K_NO_WAIT);
    zassert_true(events_after & BUTTON_EVENT, 
                 "Callback should post BUTTON_EVENT");
    
    printk("✓ Callback correctly posts event\n");
}

/**
 * @brief Test callback can be called multiple times
 */
ZTEST(button_press_tests, test_callback_multiple_calls)
{
    /* First press */
    button_test_callback(NULL, NULL, BIT(0));
    k_msleep(5);
    
    uint32_t events1 = k_event_wait(&button_events, BUTTON_EVENT, 
                                    false, K_NO_WAIT);
    zassert_true(events1 & BUTTON_EVENT, 
                 "First callback should post event");
    
    /* Clear and press again */
    k_event_clear(&button_events, BUTTON_EVENT);
    button_test_callback(NULL, NULL, BIT(0));
    k_msleep(5);
    
    uint32_t events2 = k_event_wait(&button_events, BUTTON_EVENT, 
                                    false, K_NO_WAIT);
    zassert_true(events2 & BUTTON_EVENT, 
                 "Second callback should also post event");
    
    printk("✓ Callback handles multiple calls\n");
}

/**
 * @brief Test callback doesn't corrupt event flags
 * 
 * Verifies that calling the callback only sets BUTTON_EVENT,
 * not other potential event bits
 */
ZTEST(button_press_tests, test_callback_event_integrity)
{
    /* Clear all events */
    k_event_clear(&button_events, 0xFFFFFFFF);
    
    /* Call callback */
    button_test_callback(NULL, NULL, BIT(0));
    k_msleep(5);
    
    /* Check that ONLY BUTTON_EVENT is set */
    uint32_t all_events = k_event_wait(&button_events, 0xFFFFFFFF, 
                                       false, K_NO_WAIT);
    zassert_equal(all_events, BUTTON_EVENT, 
                  "Only BUTTON_EVENT should be set, got 0x%08x", all_events);
    
    printk("✓ Callback maintains event integrity\n");
}

/******************************************************************************
 * PART 2: Test full button press flow with student_main()
 * These tests run the student's main() and verify end-to-end behavior
 ******************************************************************************/

/**
 * @brief Test that student_main responds to first button press
 * 
 * This tests the complete flow:
 * 1. Start student_main (which waits for button press)
 * 2. Simulate button press via callback
 * 3. Verify LED_STATE toggles
 */
ZTEST(button_press_tests, test_main_responds_to_first_press)
{
    /* Start with LED OFF */
    LED_STATE = LED_OFF;
    int initial_state = LED_STATE;
    
    /* Start student_main in background thread */
    student_main_tid = k_thread_create(&student_main_thread,
                                       student_main_stack,
                                       K_THREAD_STACK_SIZEOF(student_main_stack),
                                       student_main_thread_entry,
                                       NULL, NULL, NULL,
                                       STUDENT_MAIN_PRIORITY, 0, K_NO_WAIT);
    
    /* Give main() time to reach k_event_wait() */
    k_msleep(50);
    
    /* Verify main is waiting (LED should not have changed yet) */
    zassert_equal(LED_STATE, initial_state,
                  "LED should not change before button press");
    
    /* Simulate button press */
    printk("Simulating button press...\n");
    button_test_callback(NULL, NULL, BIT(0));
    
    /* Give main() time to process the event */
    k_msleep(100);
    
    /* Verify LED_STATE toggled */
    zassert_not_equal(LED_STATE, initial_state,
                      "LED_STATE should toggle after button press (was %d, now %d)",
                      initial_state, LED_STATE);
    
    printk("✓ Main loop responds to first button press\n");
    printk("  Initial: %s, After press: %s\n",
           initial_state ? "ON" : "OFF",
           LED_STATE ? "ON" : "OFF");
}

/**
 * @brief Test that student_main responds to second button press
 * 
 * Tests the complete flow with two presses to ensure LED toggles correctly
 */
ZTEST(button_press_tests, test_main_responds_to_second_press)
{
    /* Start with LED OFF */
    LED_STATE = LED_OFF;
    
    /* Start student_main */
    student_main_tid = k_thread_create(&student_main_thread,
                                       student_main_stack,
                                       K_THREAD_STACK_SIZEOF(student_main_stack),
                                       student_main_thread_entry,
                                       NULL, NULL, NULL,
                                       STUDENT_MAIN_PRIORITY, 0, K_NO_WAIT);
    
    k_msleep(50);
    
    /* First press - should turn LED ON */
    printk("First button press...\n");
    button_test_callback(NULL, NULL, BIT(0));
    k_msleep(100);
    
    int state_after_first = LED_STATE;
    zassert_equal(state_after_first, LED_ON,
                  "LED should be ON after first press");
    
    /* Second press - should turn LED OFF */
    printk("Second button press...\n");
    button_test_callback(NULL, NULL, BIT(0));
    k_msleep(100);
    
    int state_after_second = LED_STATE;
    zassert_equal(state_after_second, LED_OFF,
                  "LED should be OFF after second press");
    
    printk("✓ Main loop correctly handles two button presses\n");
    printk("  OFF → ON → OFF\n");
}

/**
 * @brief Test LED state toggles correctly
 * 
 * Verifies the toggle pattern: OFF → ON → OFF
 */
ZTEST(button_press_tests, test_led_toggle_pattern)
{
    LED_STATE = LED_OFF;
    
    /* Start student_main */
    student_main_tid = k_thread_create(&student_main_thread,
                                       student_main_stack,
                                       K_THREAD_STACK_SIZEOF(student_main_stack),
                                       student_main_thread_entry,
                                       NULL, NULL, NULL,
                                       STUDENT_MAIN_PRIORITY, 0, K_NO_WAIT);
    
    k_msleep(50);
    
    /* Verify initial state */
    zassert_equal(LED_STATE, LED_OFF, "LED should start OFF");
    
    /* Press 1: OFF → ON */
    button_test_callback(NULL, NULL, BIT(0));
    k_msleep(100);
    zassert_equal(LED_STATE, LED_ON, "LED should be ON after first press");
    
    /* Press 2: ON → OFF */
    button_test_callback(NULL, NULL, BIT(0));
    k_msleep(100);
    zassert_equal(LED_STATE, LED_OFF, "LED should be OFF after second press");
    
    printk("✓ LED toggle pattern correct: OFF → ON → OFF\n");
}

/******************************************************************************
 * PART 3: Edge cases and timing tests
 ******************************************************************************/

/**
 * @brief Test rapid button presses
 * 
 * Verifies that events aren't lost when presses happen quickly
 */
ZTEST(button_press_tests, test_rapid_button_presses)
{
    LED_STATE = LED_OFF;
    
    /* Start student_main */
    student_main_tid = k_thread_create(&student_main_thread,
                                       student_main_stack,
                                       K_THREAD_STACK_SIZEOF(student_main_stack),
                                       student_main_thread_entry,
                                       NULL, NULL, NULL,
                                       STUDENT_MAIN_PRIORITY, 0, K_NO_WAIT);
    
    k_msleep(50);
    
    /* First rapid press */
    button_test_callback(NULL, NULL, BIT(0));
    k_msleep(10);  /* Very short delay */
    
    /* Verify state changed */
    int state_after_first = LED_STATE;
    zassert_not_equal(state_after_first, LED_OFF,
                      "LED should have changed after rapid first press");
    
    /* Second rapid press */
    button_test_callback(NULL, NULL, BIT(0));
    k_msleep(100);  /* Give more time for second press to process */
    
    /* Verify state changed back */
    zassert_not_equal(LED_STATE, state_after_first,
                      "LED should toggle even with rapid presses");
    
    printk("✓ System handles rapid button presses\n");
}

/**
 * @brief Test event clearing
 * 
 * Verifies that events are properly cleared between presses
 */
ZTEST(button_press_tests, test_event_clearing)
{
    /* Clear events */
    k_event_clear(&button_events, BUTTON_EVENT);
    
    /* Post event */
    button_test_callback(NULL, NULL, BIT(0));
    k_msleep(5);
    
    /* Verify event is set */
    uint32_t events = k_event_wait(&button_events, BUTTON_EVENT, 
                                   false, K_NO_WAIT);
    zassert_true(events & BUTTON_EVENT, "Event should be set");
    
    /* Clear it */
    k_event_clear(&button_events, BUTTON_EVENT);
    
    /* Verify it's cleared */
    events = k_event_wait(&button_events, BUTTON_EVENT, false, K_NO_WAIT);
    zassert_false(events & BUTTON_EVENT, "Event should be cleared");
    
    printk("✓ Events properly cleared between operations\n");
}

/* Register test suite */
ZTEST_SUITE(button_press_tests, NULL, NULL, test_before, test_after, NULL);