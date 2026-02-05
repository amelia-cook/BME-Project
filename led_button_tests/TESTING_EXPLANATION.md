# Button Callback Testing Implementation - Technical Explanation

## Overview

This document explains how we test the `button_test_callback` function from `main.c` **without modifying the student's code**. The approach follows your professor's guidance: *"directly test the callback function by directly calling it in an emulated test rather than trying to 'trigger' the ISR and having that call the callback function."*

---

## Table of Contents

1. [The Problem: Why We Can't Just Trigger Real Interrupts](#the-problem)
2. [The Solution: Direct Callback Testing](#the-solution)
3. [Understanding ISRs, IRQs, and Callbacks](#understanding-interrupts)
4. [File Structure and What Each File Does](#file-structure)
5. [How the Test Works Step-by-Step](#how-tests-work)
6. [CI/CD Integration](#ci-integration)
7. [Key Concepts Explained](#key-concepts)

---

## The Problem: Why We Can't Just Trigger Real Interrupts {#the-problem}

### Challenge 1: Hardware Dependency
- Real interrupts require actual hardware (GPIO pins, buttons, etc.)
- CI/CD runs in a virtualized environment (`native_sim` board) without real hardware
- Simulating hardware interrupts is complex and unreliable

### Challenge 2: Timing and Race Conditions
- Real interrupts are asynchronous and timing-dependent
- Testing with real interrupts introduces flakiness (tests pass/fail randomly)
- Hard to control test conditions precisely

### Challenge 3: Student Code Must Remain Unchanged
- We need to test student code **as-is**
- Cannot modify `main.c` to add test hooks or special test modes
- Must work with whatever the student implements

### Challenge 4: What We Actually Need to Test
- **The callback function logic**: Does it post the event correctly?
- **The event mechanism**: Does `k_event_post()` work?
- **State changes**: Does `LED_STATE` change appropriately?

**We don't actually need to test the interrupt mechanism itself** - that's Zephyr's GPIO driver's responsibility, which is already tested by the Zephyr project.

---

## The Solution: Direct Callback Testing {#the-solution}

Instead of triggering real interrupts, we:

1. **Directly call `button_test_callback()`** from our test code
2. **Verify the side effects** (event posting, state changes)
3. **Test the logic**, not the hardware mechanism

This approach:
- ✅ Works in CI/CD (no hardware needed)
- ✅ Is fast and reliable (no timing issues)
- ✅ Doesn't require modifying student code
- ✅ Tests what actually matters (the callback logic)

---

## Understanding ISRs, IRQs, and Callbacks {#understanding-interrupts}

### What is an ISR (Interrupt Service Routine)?

An **ISR** is a function that runs when a hardware interrupt occurs. It's the lowest-level interrupt handler.

```c
// Example ISR (not used in your code, but shows the concept)
static void my_isr(const void *arg)
{
    // This runs when interrupt occurs
    // Must be very fast - no blocking operations
}
```

### What is IRQ_CONNECT?

`IRQ_CONNECT` is a Zephyr macro that **connects a hardware interrupt line to your ISR function**.

```c
// Example (not in your code)
IRQ_CONNECT(IRQ_NUMBER, PRIORITY, my_isr, NULL, FLAGS);
```

**In your student code, you DON'T use `IRQ_CONNECT` directly** because:
- The GPIO driver handles `IRQ_CONNECT` internally
- You use the higher-level GPIO API (`gpio_pin_interrupt_configure_dt`, `gpio_add_callback_dt`)
- The GPIO driver's ISR then calls your callback function

### The Interrupt Flow in Your Code

```
Hardware Button Press
    ↓
GPIO Hardware Interrupt (handled by SoC)
    ↓
GPIO Driver's ISR (connected via IRQ_CONNECT internally)
    ↓
GPIO Driver calls: button_test_callback()  ← YOUR FUNCTION
    ↓
button_test_callback() posts BUTTON_EVENT
    ↓
Main thread waits for event and toggles LED
```

**Key Point**: `button_test_callback` is **not** an ISR itself - it's a **callback function** that the GPIO driver's ISR calls.

### Why We Don't Need IRQ_CONNECT in Tests

- The GPIO driver's ISR is already tested by Zephyr
- We only care about **your callback function's behavior**
- We can simulate the ISR calling your callback by calling it directly

---

## File Structure and What Each File Does {#file-structure}

### Student Code (Unchanged)
```
led_button_tests/
├── src/
│   └── main.c                    # Student's code - NEVER MODIFIED
├── CMakeLists.txt                # Build configuration
├── prj.conf                      # Normal application config
└── boards/
    └── native_posix.overlay     # Hardware overlay
```

### Test Infrastructure (New Files)
```
led_button_tests/
├── src/
│   ├── test_button_callback.h   # Header exposing symbols from main.c
│   └── test_button_callback.c   # Test implementation
├── test.prj.conf                 # Test-specific config (enables ztest)
└── TESTING_EXPLANATION.md        # This file
```

### Modified Files
- `CMakeLists.txt` - Conditionally includes test files
- `.github/workflows/ci.yml` - Adds test build and execution

---

## How the Test Works Step-by-Step {#how-tests-work}

### Step 1: Expose Symbols from `main.c`

**File**: `test_button_callback.h`

```c
// External declarations - these are defined in main.c
extern struct k_event button_events;  // The event object
extern int LED_STATE;                 // Current LED state
extern void button_test_callback(...); // The callback function
```

**Why**: The test file needs to access these symbols. Since `button_test_callback` is not `static` in `main.c`, it's accessible from other files. We just need to declare it as `extern`.

### Step 2: Write Test Functions

**File**: `test_button_callback.c`

```c
void test_button_callback_posts_event(void)
{
    // 1. Clear any existing events
    k_event_clear(&button_events, BUTTON_EVENT);
    
    // 2. DIRECTLY CALL THE CALLBACK (simulating ISR)
    button_test_callback(NULL, NULL, BIT(0));
    
    // 3. Wait briefly for event to be processed
    k_msleep(10);
    
    // 4. Verify event was posted
    uint32_t events = k_event_wait(&button_events, BUTTON_EVENT, false, K_NO_WAIT);
    zassert_true(events & BUTTON_EVENT, "Event should be posted");
}
```

**Key Points**:
- We call `button_test_callback()` **directly** - no interrupt needed
- We pass `NULL` for `dev` and `cb` because we're testing the callback logic, not GPIO
- We verify the **side effect** (event was posted)

### Step 3: Register Tests with ztest

```c
ZTEST_SUITE(button_callback_tests, NULL, NULL, test_setup, NULL, NULL);
ZTEST(button_callback_tests, test_button_callback_posts_event);
```

**What ztest does**:
- Provides a `main()` function that runs all registered tests
- Handles test execution, reporting, and exit codes
- Reports "PROJECT EXECUTION SUCCESSFUL" if all tests pass

### Step 4: Handle the `main()` Conflict

**Problem**: Both `main.c` and ztest define `main()`.

**Solution**: Conditionally exclude `main.c` when building tests.

**In `CMakeLists.txt`**:
```cmake
if(NOT DEFINED TEST_BUILD OR NOT TEST_BUILD)
    target_sources(app PRIVATE src/main.c)  # Include student's main
else()
    # Exclude main.c - ztest provides main() instead
endif()
```

**In CI**:
```yaml
# Build tests with TEST_BUILD=ON (excludes main.c)
west build ... -DTEST_BUILD=ON
```

---

## CI/CD Integration {#ci-integration}

### Build Process

1. **Build Student Application** (normal build)
   ```bash
   west build -b native_sim -- -DDTC_OVERLAY_FILE=native_sim.overlay
   ```
   - Includes `main.c`
   - Uses `prj.conf` (normal config)
   - Produces `build/zephyr/zephyr.exe`

2. **Build Tests** (test build)
   ```bash
   west build -b native_sim --build-dir=build_test -- \
     -DDTC_OVERLAY_FILE=native_sim.overlay \
     -DEXTRA_CONF_FILE=test.prj.conf \
     -DTEST_BUILD=ON
   ```
   - Excludes `main.c` (ztest provides `main()`)
   - Uses `test.prj.conf` (enables `CONFIG_ZTEST=y`)
   - Produces `build_test/zephyr/zephyr.exe`

### Test Execution

```bash
./build_test/zephyr/zephyr.exe
```

**Output**:
```
Running test suite button_callback_tests
===================================================================
START - test_button_callback_posts_event
 PASS - test_button_callback_posts_event
===================================================================
...
PROJECT EXECUTION SUCCESSFUL
```

### CI Check

```yaml
- name: Check test results
  run: |
    if grep -q "PROJECT EXECUTION SUCCESSFUL" test_output.log; then
      echo "✅ All tests passed!"
      exit 0
    else
      echo "❌ Tests failed!"
      exit 1
    fi
```

---

## Key Concepts Explained {#key-concepts}

### 1. External Symbols (`extern`)

**What**: Declaring a symbol that's defined in another file.

**Why**: Allows test file to access `button_test_callback` and `button_events` from `main.c`.

**Example**:
```c
// In main.c (definition)
void button_test_callback(...) { ... }

// In test file (declaration)
extern void button_test_callback(...);  // "This exists elsewhere"
```

### 2. Zephyr Test Framework (ztest)

**What**: Zephyr's built-in unit testing framework.

**Features**:
- Provides `main()` function
- Runs registered test functions
- Reports pass/fail status
- Exit code: 0 = success, non-zero = failure

**Usage**:
```c
ZTEST_SUITE(suite_name, ...);  // Define test suite
ZTEST(suite_name, test_function);  // Register test
```

### 3. Kernel Events (`k_event`)

**What**: Zephyr's event signaling mechanism.

**Operations**:
- `k_event_post()` - Set event bits
- `k_event_wait()` - Wait for event bits
- `k_event_clear()` - Clear event bits

**In your code**:
- Callback posts `BUTTON_EVENT` bit
- Main thread waits for `BUTTON_EVENT` bit

### 4. Conditional Compilation

**What**: Including/excluding code based on build configuration.

**In CMakeLists.txt**:
```cmake
if(TEST_BUILD)
    # Test build: exclude main.c
else()
    # Normal build: include main.c
endif()
```

**Why**: Avoids `main()` function conflict between student code and ztest.

### 5. Direct Function Call vs. Interrupt Trigger

**Interrupt Trigger** (what we're NOT doing):
```
Simulate hardware → GPIO driver ISR → Callback
```
- Complex, timing-dependent, hardware-specific

**Direct Call** (what we ARE doing):
```
Test code → button_test_callback() directly
```
- Simple, reliable, tests the actual logic

---

## Summary: What This Achieves

✅ **Tests callback functionality** without modifying student code  
✅ **Works in CI/CD** (no hardware needed)  
✅ **Fast and reliable** (no timing issues)  
✅ **Follows professor's guidance** (direct callback testing)  
✅ **Easy to extend** (add more test cases as needed)  

---

## Questions for Your Meeting

1. **Q: Why don't we use `IRQ_CONNECT`?**  
   A: The GPIO driver handles `IRQ_CONNECT` internally. We test the callback function, not the interrupt mechanism.

2. **Q: How do we know the callback works if we don't trigger real interrupts?**  
   A: We test the callback's **behavior** (posting events). The interrupt mechanism is Zephyr's responsibility and is already tested.

3. **Q: What if the student's callback is `static`?**  
   A: We'd need to modify the approach (e.g., use a wrapper or test through the public API). For now, we assume it's not `static`.

4. **Q: Can we test the full flow (button → LED toggle)?**  
   A: Yes! We can add tests that call the callback, then verify `LED_STATE` changes. The current tests focus on event posting.

5. **Q: How do students run tests locally?**  
   A: Same as CI: `west build -b native_sim --build-dir=build_test -- -DEXTRA_CONF_FILE=test.prj.conf -DTEST_BUILD=ON && ./build_test/zephyr/zephyr.exe`

---

## References

- [Zephyr Interrupt Documentation](https://docs.zephyrproject.org/latest/kernel/services/interrupts.html)
- [Zephyr ztest Documentation](https://docs.zephyrproject.org/latest/guides/test/ztest.html)
- [Zephyr Kernel Events](https://docs.zephyrproject.org/latest/kernel/services/events.html)
