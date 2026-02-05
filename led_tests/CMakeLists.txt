# cmake_minimum_required(VERSION 3.20.0)
# find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# project(led_tests)

# target_sources(app PRIVATE src/main.c)

cmake_minimum_required(VERSION 3.20.0)

# --------------------------------------------------
# native_sim-only overlay generation
# --------------------------------------------------
if(DEFINED BOARD AND BOARD STREQUAL "native_sim")

  message(STATUS "native_sim build detected - generating devicetree overlay")

  find_package(Python3 REQUIRED COMPONENTS Interpreter)

  execute_process(
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate_overlay.py
      ${CMAKE_CURRENT_SOURCE_DIR}/boards/native_posix.overlay
    WORKING_DIRECTORY
      ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE OVERLAY_RESULT
  )

  if(NOT OVERLAY_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to generate native_sim overlay")
  endif()
  
  set(EXTRA_DTC_OVERLAY_FILE
      ${CMAKE_CURRENT_SOURCE_DIR}/boards/native_sim.overlay
  )

endif()

# --------------------------------------------------
# Standard Zephyr application setup
# --------------------------------------------------
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(led_tests)

target_sources(app PRIVATE src/main.c)
