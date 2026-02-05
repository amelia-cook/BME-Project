cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(led_button_tests)

# Conditionally include files based on build type
# When TEST_BUILD=ON, we're building tests (exclude main.c, include test files)
# When TEST_BUILD is not set, we're building normal app (include main.c, exclude test files)
if(DEFINED TEST_BUILD AND TEST_BUILD)
    # Test build: include test files, exclude student's main.c
    target_sources(app PRIVATE src/test_button_callback.c)
    message(STATUS "Building test suite (TEST_BUILD enabled - excluding main.c, ztest provides main)")
else()
    # Normal build: include student's main.c, exclude test files
    target_sources(app PRIVATE src/main.c)
    message(STATUS "Building application (normal mode - including main.c)")
endif()
