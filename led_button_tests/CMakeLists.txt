cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(led_button_tests)

# Include test files
target_sources(app PRIVATE src/test_button_callback.c)

# Include main.c only if not building with ztest (which provides its own main)
# We check this by looking for a custom variable TEST_BUILD
# When TEST_BUILD=ON, we're building tests and exclude main.c
if(NOT DEFINED TEST_BUILD OR NOT TEST_BUILD)
    target_sources(app PRIVATE src/main.c)
    message(STATUS "Building application (normal mode - including main.c)")
else()
    message(STATUS "Building test suite (TEST_BUILD enabled - excluding main.c, ztest provides main)")
endif()
