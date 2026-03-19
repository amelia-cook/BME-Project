cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(led_blink_tests)

if(DEFINED TEST_BUILD AND TEST_BUILD)

    set(MAIN_C_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")

    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/main_student.c
"#define main student_main
#include \"${MAIN_C_PATH}\"
#undef main
")

    target_sources(app PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/main_student.c
        src/test_led_blink.c
    )

    target_include_directories(app PRIVATE src)

    message(STATUS "Building LED blink test suite (main() renamed to student_main())")

else()

    target_sources(app PRIVATE src/main.c)
    message(STATUS "Building LED blink application")

endif()