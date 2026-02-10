cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(led_button_tests)

if(DEFINED TEST_BUILD AND TEST_BUILD)
    # Test build: Create a wrapper that includes main.c but renames main()
    # This avoids conflict with ztest's main()
    
    set(MAIN_C_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")
    
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/main_student.c
"#define main student_main
#include \"${MAIN_C_PATH}\"
#undef main
")
    
    target_sources(app PRIVATE 
        ${CMAKE_CURRENT_BINARY_DIR}/main_student.c
        src/test_button_callback.c
    )
    
    message(STATUS "Building test suite (main() renamed to student_main)")
else()
    # Normal build
    target_sources(app PRIVATE src/main.c)
    message(STATUS "Building application")
endif()





# Basic Setup:

# Sets minimum CMake version to 3.20.0
# Finds and loads the Zephyr RTOS package
# Names the project "led_button_tests"

# Conditional Build Logic:
# The file uses an if statement to check whether TEST_BUILD is defined and set to true, then builds differently based on this:
# Test Build Mode (when TEST_BUILD is defined):

# Creates a clever workaround for a naming conflict: test frameworks like Zephyr's ztest have their own main() function, which would collide with your application's main()
# Generates a wrapper file (main_wrapper.c) on-the-fly that:

# Renames your original main() to student_main() using a preprocessor macro
# Includes your actual main.c file
# Undefines the macro afterward


# Adds both the wrapper and the test file (test_button_callback.c) to the build
# This allows the test framework's main() to run and call your renamed student_main() when needed

# Normal Build Mode (default):

# Simply includes your main.c file as-is
# No renaming tricks needed

# This is a common pattern in embedded testing where you want to run unit tests on your code without rewriting it, while avoiding the main() function conflict.