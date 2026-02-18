[![pipeline status](https://gitlab.oit.duke.edu/EmbeddedMedicalDevices/duke-bme554-ecg-temp-ble-lab/badges/main/pipeline.svg)](https://gitlab.oit.duke.edu/EmbeddedMedicalDevices/duke-bme554-ecg-temp-ble-lab/-/commits/main) 

# Duke BME 554: ECG, Temperature, and BLE Device Labs

The CI build badge above indicates the status of the main branch of the repository. A green badge means that the latest commit has passed all tests, while a red badge indicates that there are failing tests.

## Project Repository Overview

* `application/src/main.c` - main application code
* `application/CMakePresets.json` - CMake presets file (build configuration)
* `application/CMakeLists.txt` - build system configuration file
* `application/prj.conf` - Zephyr configuration file
* `.gitlab-ci.yml` - GitLab CI configuration file
* `.gitignore` - ignore files that are not needed in the git repository
* `.west.yml` - Zephyr west configuration file
* `testing/technical_report.ipynb` - Jupyter notebook for the technical report

## Getting Started

You will need to run `west update` to download the Zephyr SDK and other
dependencies before building the project. This will also create a local Zephyr
workspace in the `external/` directory, which will take a while to download, but
only needs to be done once.

## Zephyr Devicetree, GPIO & Callbacks

First Lab to explore Zephyr's Devicetree, GPIO, and Interrupt Service Routines (ISRs).

[https://mlp6.github.io/Embedded-Medical-Devices/labs/zephyr-gpio-isr-callbacks-lab.html](https://mlp6.github.io/Embedded-Medical-Devices/labs/zephyr-gpio-isr-callbacks-lab.html)

## Timers

This lab introduces Zephyr's timer APIs, which are essential for implementing time-based functionality in embedded applications.

[https://mlp6.github.io/Embedded-Medical-Devices/labs/zephyr-timers-lab.html](https://mlp6.github.io/Embedded-Medical-Devices/labs/zephyr-timers-lab.html)

## Kernel Events & Threads

This lab explores Zephyr's threading model and event handling, which are crucial for managing concurrent tasks in embedded systems.

[https://mlp6.github.io/Embedded-Medical-Devices/labs/zephyr-threads-events-lab.html](https://mlp6.github.io/Embedded-Medical-Devices/labs/zephyr-threads-events-lab.html)

## State Machine Framework

This lab introduces the State Machine Framework (SMF) in Zephyr, which is used to manage complex state transitions in embedded applications.

[https://mlp6.github.io/Embedded-Medical-Devices/labs/zephyr-smf-lab.html](https://mlp6.github.io/Embedded-Medical-Devices/labs/zephyr-smf-lab.html)

## Analog to Digital Conversion (ADC)

This lab covers the use of ADC in Zephyr, which is essential for reading analog signals from sensors, including ECG electrodes.

[https://mlp6.github.io/Embedded-Medical-Devices/labs/zephyr-adc-lab.html](https://mlp6.github.io/Embedded-Medical-Devices/labs/zephyr-adc-lab.html)

## Pulse Width Modulation (PWM)

This lab covers the use of PWM in Zephyr, which is essential for controlling the brightness of LEDs and the speed of motors.

[https://mlp6.github.io/Embedded-Medical-Devices/labs/zephyr-pwm-lab.html](https://mlp6.github.io/Embedded-Medical-Devices/labs/zephyr-pwm-lab.html)

## Serial Communication (I2C/Sensors)

This lab introduces the use of I2C for communication with sensors, which is a common protocol in embedded systems.  We will use this to read temperature data from a sensor.

## Bluetooth Low Energy (BLE)

This lab covers the basics of Bluetooth Low Energy (BLE) communication, which is essential for wireless data transfer in embedded medical devices.

[https://mlp6.github.io/Embedded-Medical-Devices/labs/ecg-temp-ble-lab.html](https://mlp6.github.io/Embedded-Medical-Devices/labs/ecg-temp-ble-lab.html)
