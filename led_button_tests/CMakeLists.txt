cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(led_button_tests)

if(DEFINED TEST_BUILD AND TEST_BUILD)
    # Test build: Compile main.c with a preprocessor flag to skip main()
    # Add -D__ZTEST_MAIN__ which we'll check in a wrapper
    target_compile_definitions(app PRIVATE __ZTEST_MAIN__=1)
    
    # Create a wrapper that includes main.c but conditionally excludes main()
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/main_wrapper.c [[
#define main student_main  /* Rename main to student_main */
#include "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c"
#undef main
]])
    
    target_sources(app PRIVATE 
        ${CMAKE_CURRENT_BINARY_DIR}/main_wrapper.c
        src/test_button_callback.c
    )
    
    message(STATUS "Building test suite (main() renamed to student_main)")
else()
    # Normal build
    target_sources(app PRIVATE src/main.c)
    message(STATUS "Building application")
endif()
