
cmake_minimum_required(VERSION 3.20.0)

# --------------------------------------------------
# native_sim-only overlay generation
# --------------------------------------------------
if(DEFINED BOARD AND BOARD STREQUAL "native_sim")

  message(STATUS "native_sim build detected - generating devicetree overlay")

  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  
  set(INPUT_OVERLAY ${CMAKE_CURRENT_SOURCE_DIR}/boards/native_posix.overlay)
  set(LED_DATA ${CMAKE_BINARY_DIR}/led_aliases.cmake)
  
  execute_process(
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/extract_led_aliases.py
      ${INPUT_OVERLAY}
      ${LED_DATA}
    WORKING_DIRECTORY
      ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE OVERLAY_RESULT
  )
  
  if(NOT OVERLAY_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to generate native_sim overlay")
  endif()
  
  include(${LED_DATA})
  
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/native_sim.overlay.in
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/native_sim.overlay
    @ONLY
  )
  
  set(EXTRA_DTC_OVERLAY_FILE
      ${CMAKE_CURRENT_SOURCE_DIR}/boards/native_sim.overlay
  )

endif()

# --------------------------------------------------
# Standard Zephyr application setup
# --------------------------------------------------

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})  # defines 'app' target
project(gpio_tests)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE testing/test_state_machine.c)
target_sources(app PRIVATE testing/bme554_lib.c)
target_compile_definitions(app PRIVATE "main=student_main")







# # SPDX-License-Identifier: Apache-2.0

# cmake_minimum_required(VERSION 3.20.0)

# # --------------------------------------------------
# # native_sim specific build
# # --------------------------------------------------

# if(DEFINED BOARD AND BOARD STREQUAL "native_sim")

#   message(STATUS "native_sim build detected - generating devicetree overlay")

#   find_package(Python3 REQUIRED COMPONENTS Interpreter)
  
#   set(INPUT_OVERLAY ${CMAKE_CURRENT_SOURCE_DIR}/boards/native_posix.overlay)
#   set(LED_DATA ${CMAKE_BINARY_DIR}/aliases.cmake)
  
#   execute_process(
#     COMMAND
#       ${Python3_EXECUTABLE}
#       ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/extract_led_aliases.py
#       ${INPUT_OVERLAY}
#       ${LED_DATA}
#     WORKING_DIRECTORY
#       ${CMAKE_CURRENT_SOURCE_DIR}
#     RESULT_VARIABLE OVERLAY_RESULT
#   )
  
#   if(NOT OVERLAY_RESULT EQUAL 0)
#     message(FATAL_ERROR "Failed to generate native_sim overlay")
#   endif()
  
#   include(${LED_DATA})
  
#   configure_file(
#     ${CMAKE_CURRENT_SOURCE_DIR}/boards/native_sim.overlay.in
#     ${CMAKE_CURRENT_SOURCE_DIR}/boards/native_sim.overlay
#     @ONLY
#   )
  
#   set(EXTRA_DTC_OVERLAY_FILE
#       ${CMAKE_CURRENT_SOURCE_DIR}/boards/native_sim.overlay
#   )

# endif()

# # --------------------------------------------------
# # NORMAL BUILD COMMANDS
# # --------------------------------------------------

# find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
# project(gpio_lab)

# # set(NO_BUILD_TYPE_WARNING TRUE)

# # make sure to add the source code for new libraries
# target_sources(app PRIVATE src/main.c)







# # --------------------------------------------------
# # Linting Integration
# # --------------------------------------------------

# # find_program(CLANG_FORMAT clang-format)
# # find_program(PYTHON_EXECUTABLE python3)

# # if(NOT CLANG_FORMAT)
# #     message(FATAL_ERROR "clang-format not found")
# # endif()

# # if(NOT PYTHON_EXECUTABLE)
# #     message(FATAL_ERROR "python3 not found")
# # endif()

# # # Collect C source/header files
# # file(GLOB_RECURSE LINT_FILES
# #     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
# #     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
# # )

# # add_custom_target(run_lint ALL
# #     # COMMAND ${CLANG_FORMAT}
# #     #         --dry-run
# #     #         --Werror
# #     #         ${LINT_FILES}
# #     COMMAND ${CLANG_FORMAT}
# #             -i
# #             ${LINT_FILES}
# #     COMMAND ${PYTHON_EXECUTABLE}
# #             ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/style.py
# #             ${CMAKE_CURRENT_SOURCE_DIR}/src
# #     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
# #     COMMENT "Running style checks (naming + clang-format)"
# # )
