# Testing Quick Reference Guide

## Overview
This project tests `button_test_callback` from `main.c` **without modifying student code** by directly calling the callback function.

## Key Files

| File | Purpose |
|------|---------|
| `src/main.c` | Student code (DO NOT MODIFY) |
| `src/test_button_callback.h` | Exposes symbols from main.c for testing |
| `src/test_button_callback.c` | Test implementation using ztest |
| `test.prj.conf` | Test configuration (enables ztest) |
| `CMakeLists.txt` | Conditionally includes test files |

## How It Works

1. **Test file calls `button_test_callback()` directly** (simulating ISR)
2. **Verifies side effects** (event posted, state changed)
3. **No hardware interrupts needed**

## Building and Running Tests

### Locally
```bash
cd led_button_tests
west build -b native_sim --build-dir=build_test -- \
  -DDTC_OVERLAY_FILE=native_sim.overlay \
  -DEXTRA_CONF_FILE=test.prj.conf \
  -DTEST_BUILD=ON

./build_test/zephyr/zephyr.exe
```

### In CI
Tests run automatically on `git push` via GitHub Actions.

## Test Output

**Success**: `PROJECT EXECUTION SUCCESSFUL`  
**Failure**: `PROJECT EXECUTION FAILED` or test assertion failures

## Key Concepts

- **ISR**: Interrupt Service Routine (low-level interrupt handler)
- **IRQ_CONNECT**: Connects hardware interrupt to ISR (handled by GPIO driver)
- **Callback**: Function called by driver's ISR (what we test)
- **ztest**: Zephyr test framework (provides `main()` for tests)
- **extern**: Declares symbol defined in another file

## Why Direct Callback Testing?

✅ No hardware needed  
✅ Fast and reliable  
✅ Tests actual logic  
✅ Doesn't modify student code  
✅ Follows professor's guidance  

See `TESTING_EXPLANATION.md` for detailed explanation.
